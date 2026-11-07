// A dark cloud that rains on and off. Rain puts her out like the sea does.
// A few drops first, so there's time to get out from under it.

fn init(self) {
    self.t = self.delay or 0
    self.depth = self.depth or 160
}

fn update(self, dt) {
    let e = self.id
    self.t += dt
    let k = self.t % (self.rain + self.dry)
    let raining = k < self.rain
    let warning = not raining and k > self.rain + self.dry - 0.6
    emit(e, raining)
    if warning and rand(0, 1) < 0.3 {
        burst("rain", x(e) + rand(-18, 18), y(e) + 4, 1)
    }
    if not raining {
        return
    }
    let p = player()
    if p != nil and abs(x(p) - x(e)) < 20 and y(p) > y(e) and y(p) < y(e) + self.depth {
        kill_player("water")
    }
}
