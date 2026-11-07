// the save point at the start of each world. Lights up, becomes the place
// you come back to, and writes the game to disk.
//
// It counts as soon as Lueur goes past it, jumping over included: touching
// the little flame isn't the point, getting there is.

fn light_up(self) {
    self.lit = true
    let e = self.id
    anim(e, "lit")
    light(e, 72)
    emit(e, true)
    checkpoint(x(e), y(e))
    add_arrows(6)
    save_game()
    sfx("save", 0.7)
    burst("save", x(e), y(e) - 22)
    say("partie sauvegardée", 2)
}

fn update(self, dt) {
    let p = player()
    if self.lit or p == nil {
        return
    }
    // past it sideways, and roughly on the same floor: up to 8 tiles above,
    // not from a ledge below
    let above = y(self.id) - y(p)
    if abs(x(p) - x(self.id)) < 10 and above > -16 and above < 128 {
        light_up(self)
    }
}

fn touch(self, other) {
    if not self.lit and is_player(other) {
        light_up(self)
    }
}
