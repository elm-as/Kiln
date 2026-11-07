// A spinning blade going back and forth along (dx, dy). Nothing stops it.

fn init(self) {
    self.ox = x(self.id)
    self.oy = y(self.id)
    self.t = self.phase or 0
}

fn update(self, dt) {
    let e = self.id
    let len = sqrt(self.dx * self.dx + self.dy * self.dy)
    if len == 0 or self.speed <= 0 {
        return
    }
    self.t += dt
    let k = (1 - cos(self.t * self.speed / len * pi)) / 2
    let tx = self.ox + self.dx * k
    let ty = self.oy + self.dy * k
    set_vel(e, (tx - x(e)) / dt, (ty - y(e)) / dt)
    if rand(0, 1) < 0.1 {
        burst("spark", x(e), y(e) + 6, 1)
    }
}

fn touch(self, other) {
    if is_player(other) {
        kill_player()
    }
}

fn hit(self, dx, dy) {
    sfx("hit", 0.4, 1.8)
    return true
}
