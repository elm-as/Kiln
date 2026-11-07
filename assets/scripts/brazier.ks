// checkpoint: light it and that's where you come back after dying.
// Like the save torch, going past it is enough, jumping over it counts.
// With a link it also sets that flag, which is how the Froid knows.

fn light_up(self) {
    self.lit = true
    let e = self.id
    if self.link {
        set_flag(self.link, true)
    }
    anim(e, "lit")
    light(e, self.radius or 64)
    emit(e, true)
    sfx("checkpoint", 0.7)
    burst("flame_burst", x(e), y(e) - 10)
    // a furnace up on a shelf powers something, it's no place to come back to
    if self.machine {
        say(self.msg or "la machine se réveille", 2)
        return
    }
    checkpoint(x(e), y(e))
    if add_arrows(4) > 0 {
        say("le feu se souviendra de toi · flèches +", 2)
    } else {
        say("le feu se souviendra de toi", 2)
    }
}

fn update(self, dt) {
    let p = player()
    if self.lit or p == nil {
        return
    }
    let above = y(self.id) - y(p)
    if abs(x(p) - x(self.id)) < 10 and above > -16 and above < 128 {
        light_up(self)
    }
}

// a fire arrow lights it from afar
fn hit(self, dx, dy) {
    if self.lit or not has_power("feu") {
        return false
    }
    light_up(self)
    return true
}

fn touch(self, other) {
    if not self.lit and is_player(other) {
        light_up(self)
    }
}
