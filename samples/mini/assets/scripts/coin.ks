// bobs where it was put, and goes to whoever touches it first

fn init(self) {
    self.base = y(self.id)
    self.t = rand(0, 6)
}

fn update(self, dt) {
    self.t += dt
    set_pos(self.id, x(self.id), self.base + sin(self.t * 3) * 2)
}

fn touch(self, other) {
    if self.taken or not is_player(other) {
        return
    }
    self.taken = true
    collect()
    sfx("coin", 0.7, rand(0.95, 1.1))
    burst("sparkle", x(self.id), y(self.id))
    destroy(self.id)
}
