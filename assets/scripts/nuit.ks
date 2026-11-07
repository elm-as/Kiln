// The Night, the one who pushed the Froid. Made of dark, so nothing touches
// it until a burst of light pins it down: stunned, it can be hit by arrows,
// spear and dash. A third of its life per phase. Phase two takes the floor
// away and it starts to swoop, phase three is just angrier. Right after a
// stun it shrugs off the next burst for a moment. At the end, the light wins.

fn init(self) {
    self.ox = x(self.id)
    self.oy = y(self.id)
    self.t = 0
    self.clock = 0
    self.next = 2.2
    self.attacks = 0
    self.phase = 1
    self.state = "dark"
    self.stun = 0
    self.cool = 0
    self.immune = 0
    self.max_hp = self.hp
    boss_bar("la Nuit", 1)
}

fn bar(self) {
    boss_bar("la Nuit", self.hp / self.max_hp)
}

// heads for (tx, ty) at no more than speed, so it never jumps across the room
fn glide_to(self, tx, ty, speed) {
    let e = self.id
    let vx = (tx - x(e)) * 4
    let vy = (ty - y(e)) * 4
    let v = sqrt(vx * vx + vy * vy)
    if v > speed {
        vx = vx / v * speed
        vy = vy / v * speed
    }
    set_vel(e, vx, vy)
}

fn summon(self) {
    let e = self.id
    // a slot per shade, so they don't pile up
    let made = 0
    if not alive(self.s1) and made < self.phase {
        self.s1 = spawn("ombre", x(e) + rand(-40, 40), y(e) + 20)
        made += 1
    }
    if not alive(self.s2) and made < self.phase {
        self.s2 = spawn("ombre", x(e) + rand(-40, 40), y(e) + 20)
        made += 1
    }
    if not alive(self.s3) and made < self.phase {
        self.s3 = spawn("ombre", x(e) + rand(-40, 40), y(e) + 20)
        made += 1
    }
    burst("shadow_mist", x(e), y(e), 20)
}

fn attack(self) {
    let e = self.id
    let p = player()
    self.attacks += 1
    sfx("nuit_attack", 0.6, rand(0.9, 1.1))
    if self.phase >= 2 and self.attacks % 4 == 0 {
        // swoops at where she stands, then drifts back up
        self.state = "swoop"
        self.swoop_t = 0
        self.sx = x(p)
        self.sy = y(p) - 8
        sfx("nuit_roar", 0.7, 0.9)
        burst("shadow_mist", x(e), y(e), 16)
    } elif self.attacks % 3 == 0 {
        summon(self)
    } else {
        spawn("trait_ombre", x(e), y(e) + 10)
        if self.phase >= 2 {
            spawn("trait_ombre", x(e) - 20, y(e))
            spawn("trait_ombre", x(e) + 20, y(e))
        }
        if self.phase >= 3 {
            spawn("trait_ombre", x(e) - 40, y(e) - 6)
            spawn("trait_ombre", x(e) + 40, y(e) - 6)
        }
    }
}

fn update(self, dt) {
    let e = self.id
    self.t += dt
    self.cool -= dt
    self.immune -= dt
    cam_look(x(e), y(e))

    if self.state == "dying" {
        set_vel(e, 0, 0)
        if rand(0, 1) < 0.6 {
            burst("eclat", x(e) + rand(-24, 24), y(e) + rand(-20, 20), 4)
        }
        if self.t > 3 {
            burst("eclat", x(e), y(e), 60)
            shake(6)
            sfx("eclat", 1, 0.6)
            destroy(e)
            finish_world()
        }
        return
    }

    if self.state == "stunned" {
        self.stun -= dt
        // sinks to where the spear can reach, shaking
        glide_to(self, x(e), self.oy + self.drop + sin(self.t * 20) * 2, 160)
        if rand(0, 1) < 0.3 {
            burst("eclat", x(e) + rand(-20, 20), y(e) + rand(-16, 16), 1)
        }
        if self.stun <= 0 {
            self.state = "dark"
            self.immune = 2.5
            anim(e, "idle")
            sfx("nuit_roar", 0.6, 1.2)
        }
        return
    }

    if self.state == "swoop" {
        self.swoop_t += dt
        glide_to(self, self.sx, self.sy, 230)
        if rand(0, 1) < 0.5 {
            burst("shadow_mist", x(e), y(e), 2)
        }
        if self.swoop_t > 1.1 {
            self.state = "dark"
        }
        return
    }

    // drifts in a lazy figure eight, faster with every phase. Its own clock
    // only runs while it drifts, so after a stun it picks up where it was
    self.clock += dt
    let speed = 0.5 + self.phase * 0.15
    let tx = self.ox + sin(self.clock * speed) * self.range
    let ty = self.oy + sin(self.clock * speed * 2) * 18
    glide_to(self, tx, ty, 150)

    self.next -= dt
    if self.next <= 0 and player() != nil {
        attack(self)
        self.next = max(0.9, 2.4 - self.phase * 0.5)
    }
}

fn eclat(self) {
    if self.state == "dying" or self.state == "stunned" {
        return
    }
    if self.immune > 0 {
        burst("shadow_mist", x(self.id), y(self.id), 12)
        say("elle résiste encore à la lumière… attends un peu", 2)
        return
    }
    self.state = "stunned"
    // shorter every phase
    self.stun = self.stun_time - (self.phase - 1) * 0.6
    anim(self.id, "stun")
    flash(self.id)
    sfx("nuit_roar", 0.8, 0.7)
    // bits of light fall off her, enough to shoot back with
    add_arrows(3)
    say("la lumière la cloue sur place ! frappe !", 2)
}

fn hit(self, dx, dy) {
    let e = self.id
    if self.state == "dying" {
        return false
    }
    if self.state != "stunned" {
        // goes straight through, it's only shadow
        if not self.told {
            self.told = true
            say("rien ne l’atteint dans le noir… un éclat de lumière !", 3)
        }
        return false
    }
    if self.cool > 0 {
        return true
    }
    self.cool = 0.3
    self.hp -= 1
    flash(e)
    hitstop(0.06)
    shake(2)
    sfx("nuit_roar", 0.7, 1.4)
    burst("eclat", x(e), y(e), 10)
    bar(self)
    if self.hp <= 0 {
        self.state = "dying"
        self.t = 0
        collide(e, false)
        boss_bar()
        talk("nuit_vaincue")
        return true
    }
    // a third of its life gone: a new phase, and it shakes free
    let phase = 1
    if self.hp <= self.max_hp * 2 / 3 {
        phase = 2
    }
    if self.hp <= self.max_hp / 3 {
        phase = 3
    }
    if phase > self.phase {
        self.phase = phase
        self.state = "dark"
        self.immune = 2.5
        anim(e, "idle")
        set_flag("phase" + self.phase, true)
        shake(4)
        sfx("nuit_roar", 1, 0.5)
        if self.phase == 2 {
            say("le sol se dérobe ! il ne reste que les ombres…", 3)
        } else {
            say("la Nuit enrage !", 2)
        }
    }
    return true
}

fn dashed(self, dx, dy) {
    return hit(self, dx, dy)
}

fn touch(self, other) {
    if (self.state == "dark" or self.state == "swoop") and is_player(other) {
        kill_player("shadow")
    }
}
