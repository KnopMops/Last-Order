-- note.lua — записка на стене. Включите у объекта Script -> Interactable.
exports = { text = "На обороте нацарапано: код люка 4815" }

function on_interact(self)
  game.subtitle(self.params.text, 6)
  game.play_sound("audio/sfx/sfx_notebook_flip.wav", 0.6)
end
