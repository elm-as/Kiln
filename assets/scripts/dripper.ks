// a wet stalactite in the volcano. lets a drop go every period seconds
fn init(self) {
    self.t = self.delay
}

fn update(self, dt) {
    self.t += dt
    if self.t >= self.period {
        self.t = 0
        spawn("goutte", x(self.id), y(self.id) + 4)
    }
}
