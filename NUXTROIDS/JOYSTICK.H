/* Standard analog PC gameport joystick 1. Digital proprietary protocols excluded. */
static void joy_off(void) {
    joyenabled = joyleft = joyright = joyup = joydown = joyfire = joyedge = joyold = joyfail = 0;
}
static void joy_poll(void) {
    unsigned r[3];
    joyleft = joyright = joyup = joydown = joyfire = joyedge = 0;
    if (!joyread(r)) {
        if (++joyfail >= 8)
            joy_off();
        return;
    }
    joyfail = 0;
    joyleft = r[0] < joyxmin;
    joyright = r[0] > joyxmax;
    joyup = r[1] < joyymin || (r[2] & 2);
    joydown = r[1] > joyymax;
    joyfire = r[2] & 1;
    joyedge = joyfire && !(joyold & 1);
    joyold = r[2];
}
static unsigned median(unsigned a, unsigned b, unsigned c) {
    unsigned t;
    if (a > b) {
        t = a;
        a = b;
        b = t;
    }
    if (b > c) {
        t = b;
        b = c;
        c = t;
    }
    if (a > b)
        b = a;
    return b;
}
static void joy_calibrate(void) {
    static const char *step[3] = {"CENTER STICK - PRESS BUTTON 1", "HOLD UP-LEFT - PRESS BUTTON 1",
                                  "HOLD DOWN-RIGHT - BUTTON 1"};
    unsigned r[3], b[3], c[3], values[3][2], last = ticks, start;
    int stage = 0, released = 0, fail = 0, cancel = 0;
    const char *error = 0;
    joy_off();
    silence();
    wipe();
    centered(height / 2 - 40 * scale, "JOYSTICK CALIBRATION", 15);
    centered(height / 2 + 25 * scale, "ESC CANCELS - KEYBOARD STILL WORKS", 9);
    centered(height / 2, step[0], 12);
    while (stage < 3) {
        if (edge(1)) {
            cancel = 1;
            break;
        }
        if (ticks == last)
            continue;
        last = ticks;
        if (!joyread(r)) {
            if (++fail >= 8) {
                error = "NO GAMEPORT JOYSTICK RESPONSE";
                break;
            }
            continue;
        }
        fail = 0;
        if (!(r[2] & 1))
            released = 1;
        if ((r[2] & 1) && released) {
            if (!joyread(b) || !joyread(c))
                continue;
            values[stage][0] = median(r[0], b[0], c[0]);
            values[stage][1] = median(r[1], b[1], c[1]);
            centered(height / 2, step[stage], 12);
            ++stage;
            released = 0;
            if (stage < 3)
                centered(height / 2, step[stage], 12);
        }
    }
    if (!error && !cancel) {
        if (values[0][0] < values[1][0] + 8 || values[2][0] < values[0][0] + 8 ||
            values[0][1] < values[1][1] + 8 || values[2][1] < values[0][1] + 8)
            error = "AXES DID NOT REACH ALL POSITIONS";
        else {
            joyxmin = values[0][0] - (values[0][0] - values[1][0]) / 3;
            joyxmax = values[0][0] + (values[2][0] - values[0][0]) / 3;
            joyymin = values[0][1] - (values[0][1] - values[1][1]) / 3;
            joyymax = values[0][1] + (values[2][1] - values[0][1]) / 3;
            joyenabled = 1;
            joyold = 3;
        }
    }
    if (error) {
        wipe();
        centered(height / 2, error, 15);
        centered(height / 2 + 12 * scale, "KEYBOARD CONTROLS REMAIN ACTIVE", 9);
        start = ticks;
        while ((unsigned)(ticks - start) < 55)
            if (edge(1))
                break;
    }
    redraw = resetclock = 1;
}
