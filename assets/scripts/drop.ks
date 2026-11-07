// one drop of water. Harmless to anything but a flame.
fn init(self) {
    self.life = 0
}

fn pop(self) {
    burst("splash", x(self.id), y(self.id))
    destroy(self.id)
}

fn update(self, dt) {
    self.life += dt
    if on_ground(self.id) {
        sfx("splash", 0.25, rand(0.9, 1.2))
        pop(self)
    } elif self.life > 5 {
        destroy(self.id)
    }
}

fn touch(self, other) {
    if is_player(other) {
        kill_player("water")
        pop(self)
    }
}

// the spear bursts it before it lands
fn hit(self, dx, dy) {
    pop(self)
    return true
}
