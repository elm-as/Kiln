// Piédestal de relique ancienne : confère une Relique de Cendre permanente à Lueur.

fn init(self) {
    self.relic_id = self.relic_id or "coeur_cendre"
    self.name = self.name or "Relique"
    self.taken = relic_unlocked(self.relic_id)
}

fn touch(self, other) {
    if self.taken or not is_player(other) {
        return
    }
    self.taken = true
    relic_unlock(self.relic_id)
    sfx("checkpoint")
    shake(1.5)
    let e = self.id
    burst("sparkle", x(e), y(e) - 10, 35)
    burst("save", x(e), y(e) - 10, 25)
    say("RELIQUE OBTENUE : " + self.name, 3.0)
    save_game()
}
