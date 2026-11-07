// A slab of floor that holds until its link flag goes on, then shakes and
// drops into the dark. The last fight pulls the floor away like this.

fn init(self) {
    self.state = "hold"
    self.t = 0
}

fn update(self, dt) {
    let e = self.id
    if self.state == "hold" {
        if flag(self.link) {
            self.state = "shake"
            self.t = rand(0, 0.5)
        }
        return
    }
    self.t += dt
    if self.state == "shake" {
        set_pos(e, x(e) + rand(-1, 1), y(e))
        if self.t > 1 {
            self.state = "fall"
            self.t = 0
            collide(e, false)
            burst("rubble", x(e), y(e) + 8, 4)
        }
    } elif self.state == "fall" {
        set_vel(e, 0, min(320, vel_y(e) + 700 * dt))
        if self.t > 1.5 {
            destroy(e)
        }
    }
}
