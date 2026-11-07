// A column of rising air, w wide and h tall above where it's placed.
// Carries Lueur up. With a link it only blows while that flag is on,
// which is what a windmill sets.

fn init(self) {
    self.w = self.w or 32
    self.h = self.h or 96
}

fn blowing(self) {
    if self.link {
        return flag(self.link)
    }
    return true
}

fn update(self, dt) {
    if not blowing(self) {
        return
    }
    let e = self.id
    // streaks going up, more of them for a bigger column
    if rand(0, 1) < self.w * self.h / 9000 {
        burst("wind_up", x(e) + rand(-self.w / 2, self.w / 2), y(e) - rand(0, self.h), 1)
    }
    let p = player()
    if p != nil and abs(x(p) - x(e)) < self.w / 2 and y(p) < y(e) + 4 and y(p) > y(e) - self.h {
        // eases off near the top so she floats there instead of bouncing
        let k = min(1, (y(p) - (y(e) - self.h)) / 32)
        wind(0, -self.power * k)
    }
}
