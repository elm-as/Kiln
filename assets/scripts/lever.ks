// Strike it (spear, arrow, a dash) and it flips its link flag. With a time
// it springs back on its own after that many seconds, ticking as it goes.

fn init(self) {
    self.on = false
    self.left = 0
}

fn set(self, on) {
    self.on = on
    set_flag(self.link, on)
    if on {
        anim(self.id, "on")
        sfx("lever", 0.7, 1.1)
    } else {
        anim(self.id, "off")
        sfx("lever", 0.6, 0.8)
    }
    burst("spark", x(self.id), y(self.id) - 8, 5)
}

fn update(self, dt) {
    if not self.on or self.time <= 0 {
        return
    }
    let before = self.left
    self.left -= dt
    // a tick every half second, faster at the end
    let step = 0.5
    let pitch = 1
    if self.left < 1 {
        step = 0.25
        pitch = 1.3
    }
    if floor(before / step) != floor(self.left / step) {
        sfx("tick", 0.4, pitch)
    }
    if self.left <= 0 {
        set(self, false)
    }
}

fn hit(self, dx, dy) {
    if self.time > 0 {
        // a timed one only goes on, and a new hit winds it back up
        if not self.on {
            set(self, true)
        }
        self.left = self.time
        return true
    }
    set(self, not self.on)
    return true
}
