// A scrap of shadow. Drifts after Lueur once she's close, slower in her
// light, and comes apart under a burst, a spear or a fire arrow.

fn init(self) {
    self.t = rand(0, 6)
    self.range = self.range or 110
    self.speed = self.speed or 30
}

fn update(self, dt) {
    let e = self.id
    self.t += dt
    if self.dead {
        self.gone += dt
        if self.gone > 0.4 {
            destroy(e)
        }
        return
    }
    let p = player()
    if p == nil or dist(e, p) > self.range {
        set_vel(e, 0, sin(self.t * 2) * 6)
        return
    }
    let dx = x(p) - x(e)
    let dy = y(p) - 6 - y(e)
    let d = max(1, sqrt(dx * dx + dy * dy))
    let speed = self.speed
    if lit_at(x(e), y(e)) {
        speed = speed * 0.5
    }
    set_vel(e, dx / d * speed, dy / d * speed + sin(self.t * 5) * 8)
    flip(e, dx < 0)
    if rand(0, 1) < 0.2 {
        burst("shadow_mist", x(e), y(e), 1)
    }
}

fn die(self) {
    self.dead = true
    self.gone = 0
    collide(self.id, false)
    visible(self.id, false)
    burst("shadow_mist", x(self.id), y(self.id), 14)
    sfx("shade_die", 0.7, rand(0.9, 1.1))
}

fn eclat(self) {
    if not self.dead {
        die(self)
    }
}

fn hit(self, dx, dy) {
    if self.dead {
        return false
    }
    die(self)
    return true
}

fn touch(self, other) {
    if not self.dead and is_player(other) {
        kill_player("shadow")
    }
}
