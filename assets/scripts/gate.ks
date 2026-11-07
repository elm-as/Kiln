// Bars across the way, open while their link flag is on (or off, with
// invert). Won't shut on top of Lueur.

fn init(self) {
    self.open = false
}

fn wanted(self) {
    let f = flag(self.link)
    if self.invert {
        return not f
    }
    return f
}

fn update(self, dt) {
    let e = self.id
    let want = wanted(self)
    if want == self.open {
        return
    }
    if not want {
        let p = player()
        if p != nil and abs(x(p) - x(e)) < 12 and y(p) > y(e) - 48 and y(p) < y(e) + 4 {
            return
        }
    }
    self.open = want
    collide(e, not want)
    if want {
        anim(e, "open")
        sfx("gate", 0.6, 1)
    } else {
        anim(e, "shut")
        sfx("gate", 0.6, 0.8)
    }
    burst("dust", x(e), y(e), 4)
}
