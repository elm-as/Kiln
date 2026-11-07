// walks until it meets a wall or the end of the floor, then turns around.
// jumping on it or poking it with the spear squashes it, walking into it
// doesn't end well.

fn init(self) {
    self.dir = self.dir or -1
    self.speed = self.speed or 22
    self.dead = false
    self.gone = 0
}

fn update(self, dt) {
    let e = self.id
    if self.dead {
        self.gone += dt
        if self.gone > 0.5 {
            destroy(e)
        }
        return
    }

    if on_wall(e) != 0 {
        self.dir = -on_wall(e)
    }
    // look one step ahead and a bit down for floor
    if on_ground(e) and not floor_at(x(e) + self.dir * 7, y(e) + 4) {
        self.dir = -self.dir
    }
    set_vel(e, self.dir * self.speed, vel_y(e))
    flip(e, self.dir > 0)
}

fn die(self) {
    self.dead = true
    set_vel(self.id, 0, 0)
    collide(self.id, false)
    anim(self.id, "squish")
    flash(self.id)
    sfx("stomp")
    burst("goo", x(self.id), y(self.id) - 4)
    hitstop(0.04)
    shake(1.5)
}

fn touch(self, other) {
    if self.dead or not is_player(other) {
        return
    }
    if vel_y(other) > 0 and y(other) < y(self.id) - 3 {
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
