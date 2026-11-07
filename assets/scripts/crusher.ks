// A press hanging from the ceiling. Waits, shivers, slams down dy pixels,
// holds, and winds back up. Only the way down is deadly; the top is solid,
// you can ride it.

fn init(self) {
    self.oy = y(self.id)
    self.state = "up"
    self.t = self.delay or 0
}

fn update(self, dt) {
    let e = self.id
    self.t += dt
    if self.state == "up" {
        set_vel(e, 0, (self.oy - y(e)) / dt)
        if self.t > self.wait {
            self.state = "warn"
            self.t = 0
        }
    } elif self.state == "warn" {
        set_vel(e, 0, 0)
        set_pos(e, x(e), self.oy + rand(-1, 1))
        if self.t > 0.35 {
            self.state = "slam"
            self.t = 0
            set_pos(e, x(e), self.oy)
        }
    } elif self.state == "slam" {
        let target = self.oy + self.dy
        let v = min(420, (target - y(e)) / dt)
        set_vel(e, 0, v)
        if y(e) >= target - 1 {
            set_vel(e, 0, 0)
            self.state = "hold"
            self.t = 0
            shake(1.5)
            sfx("slam", 0.7, rand(0.9, 1.1))
            burst("dust", x(e) - 10, target + 2, 4)
            burst("dust", x(e) + 10, target + 2, 4)
        }
    } elif self.state == "hold" {
        set_vel(e, 0, 0)
        if self.t > 0.4 {
            self.state = "rise"
            self.t = 0
        }
    } elif self.state == "rise" {
        set_vel(e, 0, -50)
        if y(e) <= self.oy {
            self.state = "up"
            self.t = 0
        }
    }
}

fn touch(self, other) {
    if self.state == "slam" and is_player(other) and y(other) > y(self.id) + self.h - 4 {
        kill_player()
    }
}
