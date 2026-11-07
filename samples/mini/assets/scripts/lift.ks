// up and down between where it was put and (dx, dy) from there, eased at
// both ends. Moved by velocity, so whoever stands on it rides along.

fn init(self) {
    self.ox = x(self.id)
    self.oy = y(self.id)
    self.t = 0
}

fn update(self, dt) {
    self.t += dt
    let len = sqrt(self.dx * self.dx + self.dy * self.dy)
    let k = (1 - cos(self.t * self.speed / len * pi)) / 2
    let tx = self.ox + self.dx * k
    let ty = self.oy + self.dy * k
    set_vel(self.id, (tx - x(self.id)) / dt, (ty - y(self.id)) / dt)
}
