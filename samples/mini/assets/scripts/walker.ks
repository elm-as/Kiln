// walks until a wall or the edge of its floor, then turns round. Land on
// it to squash it; walk into it and you're sent back to the start.

fn update(self, dt) {
    let e = self.id
    let ahead = x(e) + self.dir * 7
    if on_wall(e) != 0 or not floor_at(ahead, y(e) + 2) {
        self.dir = -self.dir
    }
    set_vel(e, self.dir * self.speed, vel_y(e))
    flip(e, self.dir > 0)
}

fn touch(self, other) {
    if not is_player(other) {
        return
    }
    if vel_y(other) > 0 and y(other) < y(self.id) - 3 {
        set_vel(other, vel_x(other), -220)
        sfx("stomp", 0.8)
        burst("poof", x(self.id), y(self.id) - 4)
        destroy(self.id)
    } else {
        hurt()
    }
}
