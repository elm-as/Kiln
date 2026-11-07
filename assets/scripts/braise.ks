// Braise, the old ember. Says his piece the first time you come close,
// after that he just mumbles his hint when you stand next to him.

fn init(self) {
    self.done = false
}

fn update(self, dt) {
    let p = player()
    if p == nil {
        return
    }
    let d = dist(self.id, p)
    if not self.done and d < 44 {
        self.done = true
        if self.dialogue != "" {
            talk(self.dialogue)
        }
    } elif self.done and d < 24 and self.hint != "" {
        say(self.hint)
    }
}
