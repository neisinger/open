/*
 * GestureDecoder - Detects and reports touch gestures
 *
 * Supported Gestures:
 *   1. Slide gestures: Moving finger along the touch strip
 *      - Adjusts volume, brightness, or scroll depending on function setting
 *
 *   2. Single tap: Quick touch and release without sliding
 *      - Triggers microphone mute (KEY_MIC_MUTE)
 *      - Detection: Touch held for < 300ms, no finger movement
 *
 *   3. Double tap: Two quick taps within 400ms
 *      - Triggers workstation lock (Win+L on Windows)
 *      - Detection: Second tap must occur within 400ms of first tap release
 *
 * Configuration:
 *   - Tap actions are always enabled and cannot be disabled
 *   - Slide gesture function can be configured via CLI:
 *     ssc set function volume|scroll|brightness
 *
 * Timing Constants:
 *   - TAP_MAX_DURATION: Maximum touch duration to count as a tap (300ms)
 *   - DOUBLE_TAP_WINDOW: Maximum time between taps for double-tap (400ms)
 */
class GestureDecoder : public genericTimer::Timer {

    TouchSensor* touchSensor;
    KeyReporter* keyReporter;
    DeviceConfiguration* deviceConfiguration;

    int oldFingerPos = -1;

    static const int queueSize = 4;
    signed char queue[queueSize];

    // Tap detection state
    // Timer runs every 20ms (start(2) = 2 * 10ms = 20ms)
    static const int TAP_MAX_DURATION = 15;    // 15 * 20ms = 300ms max touch duration for tap
    static const int DOUBLE_TAP_WINDOW = 20;   // 20 * 20ms = 400ms window for second tap
    int touchStartTime = 0;          // Timer ticks when touch began
    int lastTapTime = 0;             // Timer ticks when last tap was released
    int currentTime = 0;             // Running timer counter
    bool hasMoved = false;           // Whether finger moved during this touch
    bool isTouching = false;         // Current touch state
    bool waitingForDoubleTap = false; // Waiting for potential second tap
    bool releaseProcessed = true;    // Track if we've processed the finger release

    void onTimer() {

        currentTime++;
        checkSensor();
        checkTap();
        optimizeQueue();
        checkQueue();

        // check every 20ms
        start(2);
    }

    void checkSensor() {

        int max = 0;
        int avg = 0;
        int maxIndex = 0;
        int channelCount = touchSensor->getChannelCount();
        for (int i = 0; i < channelCount; i++) {
            int v = touchSensor->getChannel(i);
            avg += v;
            if (v > max) {
                max = v;
                maxIndex = i;
            }
        }
        avg = avg / channelCount;

        int newFingerPos;
        if (max > avg * 2) {
            newFingerPos = maxIndex;
        }
        else {
            newFingerPos = -1;
        }

        // Track touch state for tap detection
        if (newFingerPos >= 0 && !isTouching) {
            // Finger just touched
            isTouching = true;
            touchStartTime = currentTime;
            hasMoved = false;
            releaseProcessed = false;  // Reset for new touch
        } else if (newFingerPos < 0 && isTouching) {
            // Finger just released - handled in checkTap()
            isTouching = false;
        }

        if (newFingerPos != oldFingerPos && newFingerPos >= 0 && oldFingerPos >= 0) {

            int change = oldFingerPos - newFingerPos;

            if (change != 0) {
                hasMoved = true;  // Mark that finger has moved (not a tap)

                if (deviceConfiguration->data.fields.flip) {
                    change = -change;
                }
                queue[0] = change * deviceConfiguration->data.fields.scale;

            }

        }

        oldFingerPos = newFingerPos;
    }

    /*
     * checkTap - Processes tap gestures after finger release
     *
     * Logic:
     *   1. On finger release, check if it was a valid tap:
     *      - Touch duration < TAP_MAX_DURATION
     *      - No finger movement during touch
     *
     *   2. If valid tap and waiting for double-tap:
     *      - Check if within DOUBLE_TAP_WINDOW of last tap
     *      - If yes: trigger double-tap action (lock workstation)
     *      - Reset waiting state
     *
     *   3. If valid tap but not completing a double-tap:
     *      - Start waiting for potential second tap
     *      - Record tap time
     *
     *   4. If waiting for double-tap but window expired:
     *      - Trigger single-tap action (mic mute)
     *      - Reset waiting state
     */
    void checkTap() {
        // Check if finger was just released (and we haven't processed it yet)
        if (!isTouching && !releaseProcessed) {
            releaseProcessed = true;  // Mark as processed
            int touchDuration = currentTime - touchStartTime;

            // Valid tap: short duration and no movement
            if (touchDuration < TAP_MAX_DURATION && touchDuration > 0 && !hasMoved) {
                if (waitingForDoubleTap && (currentTime - lastTapTime) < DOUBLE_TAP_WINDOW) {
                    // Double tap detected - lock workstation (Win+L)
                    keyReporter->reportKey(KEY_LOCK_WORKSTATION, 1);
                    waitingForDoubleTap = false;
                } else {
                    // First tap - wait for potential second tap
                    waitingForDoubleTap = true;
                    lastTapTime = currentTime;
                }
            }
        }

        // Check if double-tap window has expired without second tap
        if (waitingForDoubleTap && (currentTime - lastTapTime) >= DOUBLE_TAP_WINDOW) {
            // Single tap confirmed - mute microphone
            keyReporter->reportKey(KEY_MIC_MUTE, 1);
            waitingForDoubleTap = false;
        }
    }

    void optimizeQueue() {

        // optimize queue to move always in one direction
        // check optimize-queue.jpg

        int positives = 0;
        int negatives = 0;
        for (int i = 0; i < queueSize; i++) {
            if (queue[i] > 0) {
                positives += queue[i];
            }
            if (queue[i] < 0) {
                negatives -= queue[i];
            }
        }
        int correction = positives;
        int side = 1;
        if (positives > negatives) {
            correction = negatives;
            side = -1;
        }


        for (int i = 0; i < queueSize; i++) {
            if (queue[i] * side > 0) {
                queue[i] = 0;
            }
        }

        for (int i = queueSize - 1; i >= 0; i--) {
            int v = queue[i];
            if (v < 0) {
                v = -v;
            }

            int c = correction;
            if (c > v) {
                c = v;
            }

            queue[i] += c * side;
            correction -= c;
        }

    }

    void checkQueue() {

        // report according to the last change
        int change = queue[queueSize - 1];

        switch (deviceConfiguration->data.fields.function) {

        case DEVICE_FUNCTION_VOLUME:
            if (change > 0) {
                keyReporter->reportKey(KEY_VOLUME_UP, change);
            }
            if (change < 0) {
                keyReporter->reportKey(KEY_VOLUME_DOWN, -change);
            }

            break;

        case DEVICE_FUNCTION_BRIGHTNESS:
            if (change > 0) {
                keyReporter->reportKey(KEY_BRIGHTNESS_UP, change);
            }
            if (change < 0) {
                keyReporter->reportKey(KEY_BRIGHTNESS_DOWN, -change);
            }

            break;

        case DEVICE_FUNCTION_SCROLL:
            keyReporter->reportScroll(change);
            break;
        }

        // shift queue
        for (int i = queueSize - 2; i >= 0; i--) {
            queue[i + 1] = queue[i];
        }
        queue[0] = 0;

    }


public:
    void init(TouchSensor* touchSensor, KeyReporter* keyReporter, DeviceConfiguration* deviceConfiguration) {
        this->touchSensor = touchSensor;
        this->keyReporter = keyReporter;
        this->deviceConfiguration = deviceConfiguration;

        // give user 2 seconds to remove finger, in case he just inserted SoundSlide in the USB port
        start(200);
    }

};

