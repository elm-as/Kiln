// sleeps on the ceiling. wakes up when you come close, chases you for a
// while, gives up if you get far enough away and goes back to sleep.

fn init(self) {
    self.home_x = x(self.id)
    self.home_y = y(self.id)
    self.state = "hang"
    self.t = rand(0, 3)
    self.range = self.range or 80
    self.speed = self.speed or 48
}

fn fly_to(self, tx, ty, speed) {
    let e = self.id
    let dx = tx - x(e)
    let dy = ty - y(e)
    let d = max(1, sqrt(dx * dx + dy * dy))
    // a bit of wobble so it reads as flapping
    set_vel(e, dx / d * speed, dy / d * speed + sin(self.t * 7) * 20)
    flip(e, dx > 0)
    return d
}

fn update(self, dt) {
    let e = self.id
    let p = player()
    self.t += dt

    if self.state == "hang" {
        set_vel(e, 0, 0)
        if p != nil and dist(e, p) < self.range {
            self.state = "chase"
            anim(e, "fly")
            sfx("bat", 0.6, rand(0.9, 1.1))
        }
    } elif self.state == "chase" {
        if p == nil or dist(e, p) > self.range * 2.2 {
            self.state = "home"
        } else {
            fly_to(self, x(p), y(p) - 6, self.speed)
        }
    } elif self.state == "home" {
        let d = fly_to(self, self.home_x, self.home_y, self.speed * 0.8)
        if d < 2 {
            set_pos(e, self.home_x, self.home_y)
            self.state = "hang"
            anim(e, "hang")
        } elif p != nil and dist(e, p) < self.range * 0.8 {
            self.state = "chase"
        }
    } elif self.state == "dead" {
        set_vel(e, 0, vel_y(e) + 400 * dt)
        self.gone += dt
        if self.gone > 0.6 {
            destroy(e)
        }
    }
}

fn die(self) {
    self.state = "dead"
    self.gone = 0
    collide(self.id, false)
    anim(self.id, "hang")
    flash(self.id)
    sfx("stomp", 1, 1.3)
    burst("death", x(self.id), y(self.id), 8)
    hitstop(0.04)
}

fn touch(self, other) {
    if self.state == "dead" or not is_player(other) {
        return
    }
    // you can stomp these too, but they're harder to land on
    if vel_y(other) > 0 and y(other) < y(self.id) - 1 {
        die(self)
        bounce_player(-230)
    } else {
        kill_player()
    }
}

fn hit(self, dx, dy) {
    if self.state == "dead" {
        return false
    }
    die(self)
    return true
}
