// A plank hanging under a balloon, out of reach. Pop the balloon and it
// drops into the water at fall_to, then floats like a raft (see raft.ks),
// drifting by (dx, dy) if asked to.

fn init(self) {
    self.t = 0
    self.state = "hang"
    self.hang_y = y(self.id)
    self.balloon = spawn("ballon", x(self.id), y(self.id))
    // nothing to stand on while it's up there
    collide(self.id, false)
}

fn drift(self, dt) {
    let e = self.id
    let k = 0
    let len = sqrt(self.dx * self.dx + self.dy * self.dy)
    if len > 0 and self.speed > 0 {
        k = (1 - cos(self.t * self.speed / len * pi)) / 2
    }
    let tx = self.ox + self.dx * k
    let ty = self.oy + self.dy * k + sin(self.t * 2.2) * self.bob
    set_vel(e, (tx - x(e)) / dt, (ty - y(e)) / dt)
}

fn update(self, dt) {
    let e = self.id
    self.t += dt
    if self.state == "hang" {
        if not alive(self.balloon) {
            self.state = "fall"
            self.vy = 0
            return
        }
        let ty = self.hang_y + sin(self.t * 1.8) * 2
        set_vel(e, 0, (ty - y(e)) / dt)
        set_pos(self.balloon, x(e), ty)
    } elif self.state == "fall" {
        self.vy = min(self.vy + 500 * dt, 320)
        if y(e) + self.vy * dt < self.fall_to {
            set_vel(e, 0, self.vy)
            return
        }
        set_pos(e, x(e), self.fall_to)
        set_vel(e, 0, 0)
        burst("splash", x(e) - 8, self.fall_to + 4)
        burst("splash", x(e) + 8, self.fall_to + 4)
        sfx("splash", 0.9, 0.8)
        shake(1.5)
        collide(e, true)
        self.state = "float"
        self.ox = x(e)
        self.oy = self.fall_to
        self.t = 0
    } else {
        drift(self, dt)
    }
}
