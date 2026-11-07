// Colonne de vapeur thermique : propulse Lueur en hauteur, surtout en vol plané.
// S'active en continu ou via un interrupteur / flamme (prop 'link').

fn init(self) {
    self.w = self.w or 32
    self.h = self.h or 110
    self.power = self.power or 380
    self.vent_id = steam_vent(x(self.id), y(self.id), self.w, self.h, self.power)
}

fn blowing(self) {
    if self.link {
        return flag(self.link)
    }
    return true
}

fn update(self, dt) {
    let on = blowing(self)
    vent_set_active(self.vent_id, on)
}
