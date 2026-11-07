// the end of the level

fn touch(self, other) {
    if self.lit or not is_player(other) {
        return
    }
    self.lit = true
    let e = self.id
    anim(e, "lit")
    light(e, 120)
    emit(e, true)
    sfx("hearth")
    burst("flame_burst", x(e), y(e) - 16)
    burst("flame_burst", x(e), y(e) - 16)
    shake(3)
    finish()
}
