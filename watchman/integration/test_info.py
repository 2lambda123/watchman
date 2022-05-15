# vim:ts=4:sw=4:et:
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.


import json
import os

from watchman.integration.lib import WatchmanInstance, WatchmanTestCase


@WatchmanTestCase.expand_matrix
class TestInfo(WatchmanTestCase.WatchmanTestCase):
    def test_sock_name(self) -> None:
        resp = self.watchmanCommand("get-sockname")
        self.assertEqual(
            resp["sockname"],
            WatchmanInstance.getSharedInstance().getSockPath().legacy_sockpath(),
        )

    def test_get_config_empty(self) -> None:
        root = self.mkdtemp()
        self.watchmanCommand("watch", root)
        resp = self.watchmanCommand("get-config", root)
        self.assertEqual(resp["config"], {})

    def test_get_config(self) -> None:
        config = {"test-key": "test-value"}
        root = self.mkdtemp()
        with open(os.path.join(root, ".watchmanconfig"), "w") as f:
            json.dump(config, f)
        self.watchmanCommand("watch", root)
        resp = self.watchmanCommand("get-config", root)
        self.assertEqual(resp["config"], config)
