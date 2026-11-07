// goes back and forth between where it was placed and (dx, dy) away from
// there, slowing down at both ends. Moved by velocity so whoever stands on it
// gets carried. With a link it only runs while that flag is on.

fn init(self) {
    self.ox = x(self.id)
    self.oy = y(self.id)
    self.t = self.phase or 0
}

fn update(self, dt) {
    if self.link and not flag(self.link) {
        set_vel(self.id, 0, 0)
        return
    }
    let len = sqrt(self.dx * self.dx + self.dy * self.dy)
    if len == 0 or self.speed <= 0 {
        return
    }
    let leg = len / self.speed
    self.t += dt
    let k = (1 - cos(self.t / leg * pi)) / 2
    let tx = self.ox + self.dx * k
    let ty = self.oy + self.dy * k
    set_vel(self.id, (tx - x(self.id)) / dt, (ty - y(self.id)) / dt)
}
