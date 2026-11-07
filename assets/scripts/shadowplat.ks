// Only there in the light. Lueur's own glow, a lantern, an arrow going by,
// a burst: while any of it reaches, it's solid. Lingers a moment after, so
// the edge of her light doesn't flicker it out from under her.

fn init(self) {
    self.on = true
    self.left = 0
    self.ox = x(self.id)
    self.oy = y(self.id)
    self.t = self.phase or 0
}

fn update(self, dt) {
    let e = self.id
    // some drift, same as the forge platforms
    let len = sqrt(self.dx * self.dx + self.dy * self.dy)
    if len > 0 and self.speed > 0 {
        self.t += dt
        let k = (1 - cos(self.t * self.speed / len * pi)) / 2
        set_vel(e, (self.ox + self.dx * k - x(e)) / dt, (self.oy + self.dy * k - y(e)) / dt)
    }

    if lit_at(x(e), y(e)) {
        self.left = 0.35
    } else {
        self.left -= dt
    }
    let on = self.left > 0
    if on == self.on {
        return
    }
    self.on = on
    collide(e, on)
    if on {
        anim(e, "solid")
        burst("shadow_mist", x(e), y(e) + 2, 3)
    } else {
        anim(e, "faint")
    }
}
