// Ice that can't take her heat. Stand on it or lean on it and it melts
// away, then freezes back a while later, once she's out of the way.

fn init(self) {
    self.ox = x(self.id)
    self.oy = y(self.id)
    self.melt = 0
    self.gone = false
    self.t = 0
}

fn touching(self, p) {
    if ground_of(p) == self.id {
        return true
    }
    // pressed against a side: her feet somewhere along the block
    return abs(x(p) - self.ox) < 13 and y(p) > self.oy + 2 and y(p) < self.oy + 18
}

fn update(self, dt) {
    let e = self.id
    let p = player()
    if self.gone {
        self.t += dt
        let clear = p == nil or abs(x(p) - self.ox) > 14 or abs(y(p) - self.oy - 8) > 24
        if self.t > self.respawn and clear {
            self.gone = false
            self.melt = 0
            visible(e, true)
            collide(e, true)
            anim(e, "whole")
            burst("frost", self.ox, self.oy + 8, 6)
            sfx("crack", 0.35, 1.4)
        }
        return
    }

    if p != nil and touching(self, p) {
        let rate = 1.0
        if relic_equipped("souffle_chaud") {
            rate = 1.8
        }
        self.melt += dt * rate
        if rand(0, 1) < 0.25 {
            burst("drip", self.ox + rand(-6, 6), self.oy + 15, 1)
        }
    } else {
        self.melt = max(0, self.melt - dt * 0.3)
    }

    let k = self.melt / self.time
    if k >= 1 {
        self.gone = true
        self.t = 0
        visible(e, false)
        collide(e, false)
        burst("steam", self.ox, self.oy + 8)
        sfx("melt", 0.6)
    } elif k > 0.66 {
        anim(e, "m2")
    } elif k > 0.33 {
        anim(e, "m1")
    } else {
        anim(e, "whole")
    }
}
