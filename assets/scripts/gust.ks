// Wind from the side over a w by h area above where it's placed. Blows
// for "on" seconds, rests for "off" seconds, if off is set at all.

fn init(self) {
    self.w = self.w or 96
    self.h = self.h or 64
    self.t = self.delay or 0
}

fn blowing(self) {
    if self.off <= 0 {
        return true
    }
    return self.t % (self.on + self.off) < self.on
}

fn update(self, dt) {
    let e = self.id
    self.t += dt
    if not blowing(self) {
        return
    }
    if rand(0, 1) < self.w * self.h / 8000 {
        let fx = "wind_right"
        if self.push < 0 {
            fx = "wind_left"
        }
        burst(fx, x(e) + rand(-self.w / 2, self.w / 2), y(e) - rand(0, self.h), 1)
    }
    let p = player()
    if p != nil and abs(x(p) - x(e)) < self.w / 2 and y(p) < y(e) + 4 and y(p) > y(e) - self.h {
        wind(self.push, 0)
    }
}
