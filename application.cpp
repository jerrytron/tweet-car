#include "elapsedMillis/elapsedMillis.h"
#include "PhoBot/PhoBot.h"

PhoBot p = PhoBot(6.0, 6.0);

const int8_t kErrorUnknown = -1;
const int8_t kErrorQueueFull = -2;
const int8_t kErrorNoCommand = -3;
const int8_t kErrorBadCommand = -4;
const int8_t kSuccessCmdRun = 1;
const int8_t kSuccessCmdQueued = 2;

const uint8_t kCommandQueueSize = 20;
const String kDefaultSpeed = "25";
const uint8_t kDefaultDuration = 1;

bool _driving;
bool _running;
uint32_t _duration;
elapsedMillis _timeElapsed;
//
String _cmdQueue[kCommandQueueSize];
// Current index available to save a new command.
uint8_t _saveIndex;
// Current command index.
uint8_t _cmdIndex;
// Commands remaining on the queue.
uint8_t _cmdCount;


void setup() {
    Serial.begin(9600);
    
    Particle.function("command", command);
    
    _driving = false;
    _running = false;
    _saveIndex = 0;
    _cmdIndex = 0;
    _cmdCount = 0;
}

void loop() {
    if ((_running && _duration) && _timeElapsed >= _duration) {
        Serial.println("Command complete.");
        if (_cmdCount > 0) {
            Serial.println("Running next command.");
            // Run the next command.
            runNextCmd();
        } else {
            Serial.println("Command queue complete.");
            // No more commands to run.
            runCommand("stop-0");
            _running = false;
        }
    }
}

void resetCommands() {
    runCommand("!stop-0");
    _driving = false;
    _running = false;
    _cmdIndex = 0;
    _cmdCount = 0;
    _saveIndex = 0;
}

int8_t command(String aCmd) {
    Serial.println("Cmd: " + aCmd);
    
    // If from twitter, will have @egotweetcar or similar in front.
    if (aCmd.charAt(0) == '@') {
        aCmd = aCmd.substring(aCmd.indexOf(' ') + 1);
    }
    
    // Cleanup any whitespace.
    aCmd.trim();
    
    // Admin command incoming.
    if (aCmd.charAt(0) == '*') {
        return adminCommand(aCmd.substring(1));
    }
    
    // Should command run immediately?
    bool runNow = false;
    if (aCmd.charAt(0) == '!') {
        runNow = true;
        aCmd = aCmd.substring(1);
        _cmdQueue[_cmdIndex] = aCmd;
    } else {
        if (_cmdCount == kCommandQueueSize) {
            return kErrorQueueFull;
        }
        // Put the command in the queue.
        _cmdQueue[_saveIndex] = aCmd;
        _saveIndex++;
        _cmdCount++;
        
        if (_saveIndex >= kCommandQueueSize) {
            _saveIndex = 0;
            if (_cmdQueue[_saveIndex].length()) {
                Serial.println("[Warning] Cmd index not empty: " + _cmdQueue[_saveIndex]);
            }
        }
    }

    int8_t status = kSuccessCmdQueued;
    if (!_running || runNow) {
        status = runNextCmd();
    }
    return status;
}

int8_t runNextCmd() {
    if (_cmdCount == 0) {
        return kErrorNoCommand;   
    }
    // Retrieve the current command to run.
    String cmd = _cmdQueue[_cmdIndex];
    // Clear out the command now that we are running it.
    _cmdQueue[_cmdIndex] = "";
    // Move to the next command index.
    _cmdIndex++;
    if (_cmdIndex == kCommandQueueSize) {
        _cmdIndex = 0;
    }
    // One less command on the queue.
    _cmdCount--;
    
    return runCommand(cmd);
}

int8_t runCommand(String aCmd) {
    // Direction: forward, back, left, right, stop
    // Duration in Seconds: 0-100, (0 = no timer)
    // Speed: 0-100
    // Format: <direction>-<duration>-<speed>, (stop -or- stop-5 -or- forward-5-100, etc)
    
    // Get the index of the first dash (right after direction string).
    int8_t durIndex = aCmd.indexOf('-');
    String dir = "";
    String speed = kDefaultSpeed;
    uint8_t dur = kDefaultDuration;
    
    // If there is no dash, we only have a single command (stop).
    if (durIndex == -1) {
        dir = aCmd;
    } else {
        dir = aCmd.substring(0, durIndex);
        
        // Move one position past the dash to the next value.
        durIndex++;
        int8_t spdIndex = aCmd.indexOf('-', durIndex);
        // Make sure there's a next value (could have just been a stop).
        if (spdIndex > -1) {
            dur = aCmd.substring(durIndex, spdIndex).toInt();
            speed = aCmd.substring(spdIndex + 1);
        } else {
            dur = aCmd.substring(durIndex).toInt();
        }
    }
    
    // Get the return code.
    int8_t status = setMotorCmd(dir, dur, speed);
    if (status > 0) { // Was a success.
        // We are now running a command, if it wasn't already true.
        _running = true;
    }
    
    return status;
}

int8_t setMotorCmd(String aDir, uint8_t aDur, String aSpeed) {
    Serial.println("Dir: " + aDir);
    Serial.print("Dur: ");
    Serial.println(aDur);
    Serial.println("Speed: " + aSpeed);
    
    uint32_t dur = 0;
    if (aDur) {
        dur = aDur * 1000;
    }
    
    if (aDir.equalsIgnoreCase("forward")) {
        p.setMotors("M3-F-" + aSpeed);
        p.setMotors("M4-F-" + aSpeed);
        _driving = true;
    } else if (aDir.equalsIgnoreCase("back")) {
        p.setMotors("M3-B-" + aSpeed);
        p.setMotors("M4-B-" + aSpeed);
        _driving = true;
    } else if (aDir.equalsIgnoreCase("left")) {
        p.setMotors("M3-B-" + aSpeed);
        p.setMotors("M4-F-" + aSpeed);
        _driving = true;
    } else if (aDir.equalsIgnoreCase("right")) {
        p.setMotors("M3-F-" + aSpeed);
        p.setMotors("M4-B-" + aSpeed);
        _driving = true;
    } else if (aDir.equalsIgnoreCase("stop")) {
        p.setMotors("M3-S");
        p.setMotors("M4-S");
        _driving = false;
    } else {
        return kErrorBadCommand;
    }
    
    _duration = dur;
    _timeElapsed = 0;
    
    return kSuccessCmdRun;
}

int8_t adminCommand(String aCmd) {
    if (aCmd.charAt(0) == 'c') {
        // Clear command queue.
    } else if (aCmd.charAt(0) == 'x') {
        // Next driving command.
        resetCommands();
    } else if (aCmd.charAt(0) == 'n') {
        // Next driving command.
        runNextCmd();
    }
    
    return 1;
}