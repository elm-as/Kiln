// Put an arrow in the sails and it turns, or stops. While it turns it
// sets its link flag, and whatever listens to that flag goes.

fn init(self) {
    self.spinning = self.spinning or false
    set_flag(self.link, self.spinning)
    if self.spinning {
        anim(self.id, "spin")
    }
}

fn hit(self, dx, dy) {
    self.spinning = not self.spinning
    set_flag(self.link, self.spinning)
    if self.spinning {
        anim(self.id, "spin")
        sfx("windmill", 0.7, 1)
    } else {
        anim(self.id, "still")
        sfx("windmill", 0.7, 0.7)
    }
    burst("wind_up", x(self.id), y(self.id) - 20, 6)
    return true
}
