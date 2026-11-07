// The Froid. Floats over its arena behind a shield of ice while its four
// braziers are out (flags feu1 to feu4). Every one Lueur lights cracks the
// shield and makes it angrier. With all four lit the shield is gone and
// arrows and spear go through, until it breaks.

fn init(self) {
    self.ox = x(self.id)
    self.oy = y(self.id)
    self.t = 0
    self.next = 3
    self.attacks = 0
    self.lit = 0
    self.cool = 0
    self.max_hp = self.hp
    self.state = "shield"
    boss_bar("le Froid", 1)
}

fn count_lit(self) {
    let n = 0
    for i in 1..5 {
        if flag("feu" + i) {
            n += 1
        }
    }
    return n
}

fn bar(self) {
    if self.state == "shield" {
        boss_bar("le Froid", 1 - self.lit * 0.1)
    } else {
        boss_bar("le Froid", 0.6 * self.hp / self.max_hp)
    }
}

fn cracked(self) {
    let e = self.id
    flash(e)
    shake(3)
    hitstop(0.08)
    sfx("froid_hurt", 0.9)
    burst("ice_shatter", x(e), y(e), 20)
    if self.lit >= self.need {
        self.state = "weak"
        anim(e, "weak")
        self.oy += self.drop
        // shards of its own shield to shoot back at it
        add_arrows(8)
        say("son bouclier a volé en éclats ! frappe-le : flèches, ou saut et lance vers le haut", 3.5)
    } else {
        say("le Froid vacille…", 2)
    }
    bar(self)
}

fn attack(self) {
    let e = self.id
    let p = player()
    self.attacks += 1
    let kind = self.attacks % 3
    // right up close a bolt can't be dodged, so it drops shards instead
    if dist(e, p) < 70 {
        kind = 0
    }
    sfx("froid_attack", 0.6, rand(0.9, 1.1))
    if kind == 0 {
        // shards over her head, more of them the angrier it gets
        let n = 2 + floor(self.lit / 2)
        for i in 0..n {
            let off = (i - (n - 1) / 2) * 36 + rand(-8, 8)
            spawn("eclat", x(p) + off, self.ceiling)
        }
    } elif kind == 2 and self.lit >= 2 and not alive(self.bat) {
        // never more than one of its bats around
        self.bat = spawn("chauve_givre", x(e), y(e) + 20)
        burst("frost", x(e), y(e) + 20, 10)
    } else {
        spawn("trait_givre", x(e), y(e) + 12)
    }
}

fn update(self, dt) {
    let e = self.id
    self.t += dt
    self.cool -= dt

    if self.state == "dead" {
        set_vel(e, 0, 18)
        if rand(0, 1) < 0.4 {
            burst("ice_shatter", x(e) + rand(-16, 16), y(e) + rand(-16, 16), 3)
        }
        if self.t > 2.4 {
            burst("flame_burst", x(e), y(e), 40)
            burst("steam", x(e), y(e), 30)
            shake(5)
            sfx("froid_die", 1)
            spawn("portail", self.portal_x, self.portal_y)
            talk("froid_vaincu")
            destroy(e)
        }
        return
    }

    let lit = count_lit(self)
    if lit > self.lit and self.state == "shield" {
        self.lit = lit
        cracked(self)
    }

    cam_look(x(e), y(e))

    // drifts across the arena, faster the angrier it is
    let speed = 0.5 + self.lit * 0.12
    let tx = self.ox + sin(self.t * speed) * self.range
    let ty = self.oy + sin(self.t * speed * 2.3) * 12
    // eased, so dropping when the shield breaks isn't a jump
    let vx = (tx - x(e)) * 4
    let vy = (ty - y(e)) * 4
    let v = sqrt(vx * vx + vy * vy)
    if v > 150 {
        vx = vx / v * 150
        vy = vy / v * 150
    }
    set_vel(e, vx, vy)

    self.next -= dt
    if self.next <= 0 and player() != nil {
        attack(self)
        self.next = max(1.3, 3.2 - self.lit * 0.4)
    }
}

fn hit(self, dx, dy) {
    let e = self.id
    if self.state == "dead" {
        return false
    }
    if self.state == "shield" {
        sfx("crack", 0.6, 1.6)
        burst("frost", x(e), y(e), 6)
        if not self.told {
            self.told = true
            say("ça ricoche sur la glace… rallume les quatre braseros !", 3)
        }
        return true
    }
    if self.cool > 0 {
        return true
    }
    self.cool = 0.35
    self.hp -= 1
    flash(e)
    hitstop(0.06)
    shake(2)
    sfx("froid_hurt", 0.8, 1.2)
    burst("ice_shatter", x(e), y(e), 10)
    bar(self)
    if self.hp <= 0 {
        self.state = "dead"
        self.t = 0
        collide(e, false)
        boss_bar()
        set_flag("froid_mort", true)
        say("", 0)
    }
    return true
}

fn touch(self, other) {
    if self.state == "shield" and is_player(other) {
        kill_player("cold")
    }
}
