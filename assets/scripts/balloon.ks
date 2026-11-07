// holds a plank up over the water. One arrow and it's gone, the plank
// (plank.ks) notices on its own
fn hit(self, dx, dy) {
    if self.popped {
        return false
    }
    self.popped = true
    sfx("pop", 0.8, rand(0.95, 1.1))
    burst("pop", x(self.id), y(self.id) - 21)
    destroy(self.id)
    return true
}
