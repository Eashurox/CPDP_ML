from pytorch_lightning import Trainer
from examples import LightningTemplateModel
from pytorch_lightning.testing import LightningTestModel
from argparse import Namespace
from test_tube import Experiment
from pytorch_lightning.callbacks import ModelCheckpoint
import os
import shutil

import pytorch_lightning as pl
import torch
from torch.nn import functional as F
from torch.utils.data import DataLoader
from torchvision.datasets import MNIST
import numpy as np
import pdb
from . import test_models


class CoolModel(pl.LightningModule):

    def __init(self):
        super(CoolModel, self).__init__()
        # not the best model...
        self.l1 = torch.nn.Linear(28 * 28, 10)

    def forward(self, x):
        return torch.relu(self.l1(x))

    def my_loss(self, y_hat, y):
        return F.cross_entropy(y_hat, y)

    def training_step(self, batch, batch_nb):
        x, y = batch
        y_hat = self.forward(x)
        return {'training_loss': self.my_loss(y_hat, y)}

    def validation_step(self, batch, batch_nb):
        x, y = batch
        y_hat = self.forward(x)
        return {'val_loss': self.my_loss(y_hat, y)}

    def validation_end(self, outputs):
        avg_loss = torch.stack([x for x in outputs['val_loss']]).mean()
        return avg_loss

    def configure_optimizers(self):
        return [torch.optim.Adam(self.parameters(), lr=0.02)]

    @pl.data_loader
    def train_dataloader(self):
        return DataLoader(MNIST('path/to/save', train=True), batch_size=32)

    @pl.data_loader
    def val_dataloader(self):
        return DataLoader(MNIST('path/to/save', train=False), batch_size=32)

    @pl.data_loader
    def test_dataloader(self):
        return DataLoader(MNIST('path/to/save', train=False), batch_size=32)


def main():
    """
    Make sure DDP + AMP continue training correctly
    :return:
    """
    """
    Make sure DDP2 works
    :return:
    """
    hparams = test_models.get_hparams()
    model = LightningTestModel(hparams)

    save_dir = test_models.init_save_dir()

    # logger file to get meta
    logger = test_models.get_test_tube_logger(False)
    logger.log_hyperparams(hparams)
    logger.save()

    # logger file to get weights
    checkpoint = ModelCheckpoint(save_dir)

    trainer_options = dict(
        show_progress_bar=True,
        max_nb_epochs=1,
        train_percent_check=0.4,
        val_percent_check=0.2,
        checkpoint_callback=checkpoint,
        logger=logger,
        gpus=[0, 1],
        distributed_backend='dp'
    )

    # fit model
    trainer = Trainer(**trainer_options)
    result = trainer.fit(model)

    # correct result and ok accuracy
    assert result == 1, 'training failed to complete'
    pretrained_model = test_models.load_model(logger.experiment, save_dir,
                                              module_class=LightningTestModel)

    new_trainer = Trainer(**trainer_options)
    new_trainer.test(pretrained_model)

    # test we have good test accuracy
    test_models.assert_ok_test_acc(new_trainer)
    test_models.clear_save_dir()


if __name__ == '__main__':
    main()
