import os
import sys

from lightning_app import LightningApp

if __name__ == "__main__":
    sys.path.append(os.path.dirname(__file__))

    from comp.a.a import AA
    from comp.b.b import BB

    app = LightningApp(BB(AA()))
