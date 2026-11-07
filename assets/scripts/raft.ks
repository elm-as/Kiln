// Floats: bobs on the swell, drifts between where it was put and (dx, dy)
// from there, and when sink is set it goes under while someone stands on it.
// Rafts and boats both use this.

fn init(self) {
    self.ox = x(self.id)
    self.oy = y(self.id)
    self.t = self.phase or 0
    self.depth = 0
}

fn update(self, dt) {
    let e = self.id
    self.t += dt

    let k = 0
    let len = sqrt(self.dx * self.dx + self.dy * self.dy)
    if len > 0 and self.speed > 0 {
        k = (1 - cos(self.t * self.speed / len * pi)) / 2
    }

    if self.sink > 0 {
        let p = player()
        if p != nil and ground_of(p) == e {
            self.depth = min(self.depth + 9 * dt, 16)
        } else {
            self.depth = max(self.depth - 6 * dt, 0)
        }
    }

    let tx = self.ox + self.dx * k
    let ty = self.oy + self.dy * k + sin(self.t * 2.2) * self.bob + self.depth
    // moved by velocity so whoever stands on it rides along
    set_vel(e, (tx - x(e)) / dt, (ty - y(e)) / dt)
}
