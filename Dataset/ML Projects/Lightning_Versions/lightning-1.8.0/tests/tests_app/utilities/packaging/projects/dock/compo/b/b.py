from lightning_app import LightningFlow


class BB(LightningFlow):
    def __init__(self, work):
        super().__init__()
        self.work = work

    def run(self):
        self._exit()
