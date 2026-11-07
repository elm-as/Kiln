fn update(self, dt) {
    let p = player()
    if p != nil and dist(self.id, p) < 22 {
        say(self.text)
    }
}
