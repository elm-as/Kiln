// stand on it and it gives way shortly after, then comes back

fn init(self) {
    self.ox = x(self.id)
    self.oy = y(self.id)
    self.state = "idle"
    self.t = 0
}

fn update(self, dt) {
    let e = self.id
    self.t += dt

    if self.state == "idle" {
        let p = player()
        if p != nil and ground_of(p) == e {
            self.state = "shaking"
            self.t = 0
            anim(e, "cracked")
            sfx("crumble", 0.6)
        }
    } elif self.state == "shaking" {
        set_vel(e, 0, 0)
        set_pos(e, self.ox + rand(-1, 1), self.oy)
        if self.t > self.delay {
            self.state = "falling"
            self.t = 0
            set_pos(e, self.ox, self.oy)
            burst("rubble", self.ox, self.oy + 4)
        }
    } elif self.state == "falling" {
        set_vel(e, 0, min(vel_y(e) + 700 * dt, 320))
        if self.t > 0.15 {
            collide(e, false)
        }
        if self.t > 0.8 {
            visible(e, false)
            set_vel(e, 0, 0)
        }
        if self.t > self.respawn {
            self.state = "idle"
            set_vel(e, 0, 0)
            set_pos(e, self.ox, self.oy)
            visible(e, true)
            collide(e, true)
            anim(e, "whole")
            burst("dust", self.ox, self.oy)
        }
    }
}
