// Turbine à vapeur : actionnée par un geyser ou un flux d'air chaud ascendant.
// Active le drapeau 'link' lorsqu'elle tourne.

fn init(self) {
    self.angle = 0
}

fn running(self) {
    if self.vent_link {
        return flag(self.vent_link)
    }
    return true
}

fn update(self, dt) {
    let on = running(self)
    if self.link {
        set_flag(self.link, on)
    }
    if not on {
        return
    }
    let e = self.id
    if rand(0, 1) < 0.2 {
        burst("steam", x(e), y(e) - 6, 1)
    }
}
