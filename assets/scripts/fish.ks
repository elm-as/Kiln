// Waits under the surface, leaps out in an arc, dives back in. Lands on
// Lueur and she's out; the spear or a stomp gets rid of it.

fn init(self) {
    self.home_y = y(self.id)
    self.wait = self.delay or rand(0.5, 2)
    self.up = false
    self.dead = false
    self.gone = 0
    visible(self.id, false)
}

fn update(self, dt) {
    let e = self.id
    if self.dead {
        self.gone += dt
        if self.gone > 0.8 {
            destroy(e)
        }
        return
    }
    if not self.up {
        set_vel(e, 0, 0)
        set_pos(e, x(e), self.home_y)
        self.wait -= dt
        if self.wait <= 0 {
            self.up = true
            visible(e, true)
            set_gravity(e, 1)
            set_vel(e, self.drift, -self.power)
            sfx("fish", 0.4, rand(0.9, 1.1))
            burst("splash", x(e), self.home_y)
        }
        return
    }
    flip(e, vel_x(e) > 0)
    if vel_y(e) > 0 and y(e) >= self.home_y {
        self.up = false
        self.wait = self.period
        visible(e, false)
        set_gravity(e, 0)
        burst("splash", x(e), self.home_y)
    }
}

fn die(self) {
    self.dead = true
    collide(self.id, false)
    flash(self.id)
    sfx("stomp", 1, 1.4)
    burst("splash", x(self.id), y(self.id), 10)
    hitstop(0.04)
}

fn touch(self, other) {
    if self.dead or not self.up or not is_player(other) {
        return
    }
    if vel_y(other) > 0 and y(other) < y(self.id) - 2 {
        die(self)
        bounce_player(-230)
    } else {
        kill_player("water")
    }
}

fn hit(self, dx, dy) {
    if self.dead or not self.up {
        return false
    }
    die(self)
    return true
}
