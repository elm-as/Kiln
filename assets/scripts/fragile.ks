// Rotten planks nailed across a gap. The spear just knocks on them; a dash
// goes straight through.

fn dashed(self, dx, dy) {
    let e = self.id
    burst("rubble", x(e), y(e) - 16, 14)
    burst("flame_burst", x(e), y(e) - 16)
    sfx("crumble", 0.9, 0.8)
    shake(2)
    destroy(e)
    return true
}

fn hit(self, dx, dy) {
    sfx("hit", 0.4, 0.7)
    if not self.told {
        self.told = true
        say("ça ne cède pas… il faudrait foncer dedans", 2.5)
    }
    return true
}
