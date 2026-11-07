// Hangs from the ceiling until Lueur walks under it, shivers, drops.
// Grows back after a while, unless it's a one-off (the Froid throws those).

fn init(self) {
    self.ox = x(self.id)
    self.oy = y(self.id)
    self.state = "wait"
    self.t = 0
}

fn update(self, dt) {
    let e = self.id
    let p = player()
    self.t += dt
    if self.state == "wait" {
        set_vel(e, 0, 0)
        if p != nil and abs(x(p) - self.ox) < 60 and y(p) > self.oy and y(p) - self.oy < 220 {
            self.state = "shake"
            self.t = 0
            sfx("crack", 0.5, 1.2)
        }
    } elif self.state == "shake" {
        set_pos(e, self.ox + rand(-1, 1), self.oy)
        if self.t > 0.45 {
            self.state = "fall"
            self.t = 0
            set_pos(e, self.ox, self.oy)
            set_gravity(e, 1)
        }
    } elif self.state == "fall" {
        if on_ground(e) or self.t > 3 {
            burst("ice_shatter", x(e), y(e))
            sfx("shatter", 0.6, rand(0.9, 1.1))
            if self.once {
                destroy(e)
                return
            }
            self.state = "gone"
            self.t = 0
            set_gravity(e, 0)
            set_vel(e, 0, 0)
            visible(e, false)
            collide(e, false)
        }
    } elif self.state == "gone" {
        if self.t > self.regrow {
            set_pos(e, self.ox, self.oy)
            visible(e, true)
            collide(e, true)
            burst("frost", self.ox, self.oy + 4, 4)
            self.state = "wait"
        }
    }
}

fn touch(self, other) {
    if self.state == "fall" and is_player(other) {
        kill_player()
    }
}
