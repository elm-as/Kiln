// the way out of a world

fn touch(self, other) {
    if self.used or not is_player(other) {
        return
    }
    self.used = true
    sfx("portal")
    burst("portal", x(self.id), y(self.id) - 16, 40)
    shake(2)
    finish_world()
}
