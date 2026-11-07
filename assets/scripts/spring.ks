fn init(self) {
    self.cool = 0
    self.power = self.power or 430
}

fn update(self, dt) {
    self.cool -= dt
}

fn touch(self, other) {
    if not is_player(other) or self.cool > 0 or vel_y(other) < 0 {
        return
    }
    self.cool = 0.25
    bounce_player(-self.power, false)
    anim(self.id, "boing", true)
    sfx("spring")
    burst("boing", x(self.id), y(self.id) - 8)
}
