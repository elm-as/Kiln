// A ball of frost thrown at wherever Lueur was. Shatters on walls, on her,
// or on a spear or arrow in the way. The Night throws the same thing made
// of shadow: fx and how say what it's made of.

fn init(self) {
    let e = self.id
    let p = player()
    self.t = 0
    let dx = 0
    let dy = 1
    if p != nil {
        dx = x(p) - x(e)
        dy = y(p) - 6 - y(e)
    }
    let d = max(1, sqrt(dx * dx + dy * dy))
    self.vx = dx / d * self.speed
    self.vy = dy / d * self.speed
}

fn shatter(self) {
    burst(self.fx or "ice_shatter", x(self.id), y(self.id), 6)
    sfx("shatter", 0.4, rand(1.1, 1.3))
    destroy(self.id)
}

fn update(self, dt) {
    let e = self.id
    self.t += dt
    set_vel(e, self.vx, self.vy)
    if rand(0, 1) < 0.4 {
        burst(self.trail or "frost", x(e), y(e), 1)
    }
    if self.t > 5 or solid(x(e), y(e)) {
        shatter(self)
    }
}

fn touch(self, other) {
    if is_player(other) {
        kill_player(self.how or "cold")
        shatter(self)
    }
}

fn hit(self, dx, dy) {
    shatter(self)
    return true
}
