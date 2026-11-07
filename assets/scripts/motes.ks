// dust floating in the light. stays glued to the camera so it's always around
fn update(self, dt) {
    set_pos(self.id, cam_x() + 160, cam_y() + 90)
}
