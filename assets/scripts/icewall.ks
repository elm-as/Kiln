// A wall of ice across the way. Lean on it long enough and it melts, or
// put a few arrows into it from a distance. Doesn't come back.

fn init(self) {
    self.melt = 0
}

fn gone(self) {
    let e = self.id
    self.melt = -100
    burst("steam", x(e), y(e) - 24)
    burst("ice_shatter", x(e), y(e) - 24)
    sfx("melt", 0.8, 0.8)
    shake(2)
    destroy(e)
}

fn show(self) {
    let k = self.melt / self.time
    if k >= 1 {
        gone(self)
    } elif k > 0.6 {
        anim(self.id, "m2")
    } elif k > 0.3 {
        anim(self.id, "m1")
    }
}

fn update(self, dt) {
    let p = player()
    if p == nil or self.melt < 0 {
        return
    }
    let e = self.id
    if abs(x(p) - x(e)) < 14 and y(p) > y(e) - 46 and y(p) < y(e) + 2 {
        let rate = 1.0
        if relic_equipped("souffle_chaud") {
            rate = 1.8
        }
        self.melt += dt * rate
        if rand(0, 1) < 0.3 {
            burst("drip", x(e) + rand(-6, 6), y(e) - rand(4, 40), 1)
        }
        show(self)
    }
}

fn hit(self, dx, dy) {
    if self.melt < 0 {
        return false
    }
    if has_power("feu") or relic_equipped("souffle_chaud") {
        self.melt += self.time
    } else {
        self.melt += self.time * 0.35
    }
    sfx("crack", 0.6, rand(0.9, 1.1))
    burst("frost", x(self.id), y(self.id) - 24, 8)
    show(self)
    return true
}
