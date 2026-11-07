// Porte de transition Metroidvania bidirectionnelle.
// props:
// - target : nom du niveau cible (ex: "roots", "sous_bois", "hollow")
// - spawn_tag : tag du spawn d'arrivée (optionnel)
// - power_req : pouvoir requis pour franchir (optionnel: "elan", "eclat", etc.)

fn touch(self, other) {
    if self.used or not is_player(other) {
        return
    }
    if self.power_req and not has_power(self.power_req) {
        say("passage bloque...", 1.2)
        return
    }
    self.used = true
    sfx("portal")
    burst("portal", x(self.id), y(self.id) - 16, 25)
    transition_to(self.target, self.spawn_tag or "")
}
