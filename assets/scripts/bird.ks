// Flies back and forth between where it was put and dx further, bobbing.
// Same deal as a bat: land on it, spear it, shoot it, don't fly into it.

fn init(self) {
    self.ox = x(self.id)
    self.oy = y(self.id)
    self.t = self.phase or 0
}

fn update(self, dt) {
    let e = self.id
    self.t += dt
    if self.dead {
        set_vel(e, 0, vel_y(e) + 400 * dt)
        self.gone += dt
        if self.gone > 0.6 {
            destroy(e)
        }
        return
    }
    let len = max(1, abs(self.dx))
    let k = (1 - cos(self.t * self.speed / len * pi)) / 2
    let tx = self.ox + self.dx * k
    let ty = self.oy + sin(self.t * 4) * 5
    flip(e, sin(self.t * self.speed / len * pi) * self.dx < 0)
    set_vel(e, (tx - x(e)) / dt, (ty - y(e)) / dt)
}

fn die(self) {
    self.dead = true
    self.gone = 0
    collide(self.id, false)
    flash(self.id)
    sfx("stomp", 1, 1.4)
    burst("feathers", x(self.id), y(self.id), 8)
    hitstop(0.04)
}

fn touch(self, other) {
    if self.dead or not is_player(other) {
        return
    }
    if vel_y(other) > 0 and y(other) < y(self.id) - 1 {
        die(self)
        bounce_player(-240)
    } else {
        kill_player()
    }
}

fn hit(self, dx, dy) {
    if self.dead {
        return false
    }
    die(self)
    return true
}
