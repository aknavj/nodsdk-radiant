import contextlib
import io
from pathlib import Path
import re
import sys
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import config


PACK = ROOT / "gamepacks" / "SCGhostPack"
SCRIPTS = PACK / "install" / "nod" / "scripts"


class SCGhostPackTests(unittest.TestCase):
    def test_bundled_pack_is_enabled_by_default(self):
        self.assertIn("SCGhostPack", config.Config().setup_packs)

    def test_staging_is_local_repeatable_and_preserves_unrelated_files(self):
        settings = config.Config()
        settings.setup_packs = ["SCGhostPack"]
        with tempfile.TemporaryDirectory() as directory:
            with mock.patch.object(config.subprocess, "check_call") as command:
                settings.FetchGamePaks(directory)
                staged = Path(directory) / "installs" / "SCGhostPack"
                sentinel = staged / "local.txt"
                sentinel.write_text("keep", encoding="ascii")
                settings.FetchGamePaks(directory)
                command.assert_not_called()
                self.assertEqual(sentinel.read_text(encoding="ascii"), "keep")
                for source in PACK.rglob("*"):
                    if source.is_file():
                        self.assertEqual(
                            (staged / source.relative_to(PACK)).read_bytes(),
                            source.read_bytes(),
                        )

    def test_existing_external_pack_routes_are_unchanged(self):
        settings = config.Config()
        settings.setup_packs = ["Q2RePack", "Q3Pack"]
        with tempfile.TemporaryDirectory() as directory:
            with mock.patch.object(settings, "GitCloneOrUpdate") as git:
                with mock.patch.object(settings, "CheckoutOrUpdate") as svn:
                    settings.FetchGamePaks(directory)
                    git.assert_called_once_with(
                        "https://github.com/TTimo/Q2RePack.git",
                        str(Path(directory) / "installs" / "Q2RePack"),
                    )
                    svn.assert_called_once_with(
                        "svn://svn.icculus.org/gtkradiant-gamepacks/Q3Pack/trunk",
                        str(Path(directory) / "installs" / "Q3Pack"),
                    )

    def test_setup_pack_selector(self):
        with contextlib.redirect_stdout(io.StringIO()):
            settings = config.ConfigParser().parseStatements(
                None, ["setup_packs=SCGhostPack"]
            )
        self.assertEqual(settings[0].setup_packs, ["SCGhostPack"])

    def test_module_configuration_covers_required_clients(self):
        tree = ET.parse(PACK / "game" / "synapse.config")
        clients = {
            client.attrib["name"]: {
                api.attrib["name"]: api.text.strip() for api in client
            }
            for client in tree.getroot()
        }
        self.assertEqual(clients["core"], {
            "VFS": "pk3", "shaders": "quake3", "map": "mapq3",
            "eclass": "def", "surfdialog": "quake3", "image": "tga jpg png dds",
        })
        for name in ("map", "xmap", "model"):
            self.assertEqual(clients[name]["shaders"], "quake3")
        for name in ("image", "model", "shaders"):
            self.assertEqual(clients[name]["VFS"], "pk3")
        self.assertEqual(clients["shaders"]["shaders"], "quake3")

    def test_project_is_editor_only(self):
        source = (SCRIPTS / "default_project.proj").read_text(encoding="ascii")
        self.assertIn("<!DOCTYPE project", source)
        keys = {
            key.attrib["name"]: key.attrib["value"]
            for key in ET.fromstring(source)
        }
        self.assertEqual(keys["version"], "2")
        self.assertEqual(keys["brush_primit"], "0")
        self.assertEqual(keys["gamename"], "nod")
        self.assertEqual(keys["gamemode"], "sp")
        self.assertFalse(any(key.startswith("bsp_") for key in keys))
        self.assertEqual(
            keys["basepath"], "$TEMPLATEenginepath$TEMPLATEbasedir/"
        )
        self.assertEqual(
            keys["entitypath"],
            "$TEMPLATEenginepath$TEMPLATEbasedir/scripts/entities.def",
        )

    def test_entity_and_placeholder_definitions(self):
        definitions = (SCRIPTS / "entities.def").read_text(encoding="ascii")
        self.assertEqual(
            re.findall(r"/\*QUAKED (\w+)", definitions), ["worldspawn", "ghost", "misc_model"]
        )
        self.assertEqual(
            (SCRIPTS / "shaderlist.txt").read_text(encoding="ascii").split(),
            ["nod"],
        )
        shader = (SCRIPTS / "nod.shader").read_text(encoding="ascii")
        self.assertIn("textures/nod/placeholder", shader)
        self.assertIn("qer_editorimage textures/radiant/notex.png", shader)
        self.assertTrue(
            (ROOT / "install" / "base" / "textures" / "radiant" / "notex.png").is_file()
        )

    def test_room_has_six_valid_outward_wound_boxes_and_clear_spawn(self):
        source = (
            PACK / "install" / "nod" / "maps" / "test_room.map"
        ).read_text(encoding="ascii")
        source = re.sub(r"//[^\n]*", "", source)
        self.assertEqual(source.count("{"), 8)
        self.assertEqual(source.count("}"), 8)
        self.assertEqual(
            re.findall(r'"classname"\s+"([^"]+)"', source),
            ["worldspawn", "ghost"],
        )
        self.assertIn('"origin" "0 0 64"', source)
        self.assertIn('"angles" "0 0 0"', source)
        brushes = re.findall(r"\{\s*((?:\([^\n]+\n)+)\}", source)
        self.assertEqual(len(brushes), 6)
        bounds = []
        for brush in brushes:
            faces = brush.strip().splitlines()
            self.assertEqual(len(faces), 6)
            points = [
                [tuple(map(float, point.split()))
                 for point in re.findall(r"\(\s*([^()]+)\s*\)", face)]
                for face in faces
            ]
            low = tuple(min(p[axis] for face in points for p in face) for axis in range(3))
            high = tuple(max(p[axis] for face in points for p in face) for axis in range(3))
            self.assertTrue(all(low[axis] < high[axis] for axis in range(3)))
            center = tuple((low[axis] + high[axis]) / 2 for axis in range(3))
            normals = set()
            for face, (a, b, c) in zip(faces, points):
                self.assertRegex(
                    face, r"\)\s+nod/placeholder 0 0 0 1 1 0 0 0$"
                )
                u = tuple(a[axis] - b[axis] for axis in range(3))
                v = tuple(c[axis] - b[axis] for axis in range(3))
                normal = (
                    u[1] * v[2] - u[2] * v[1],
                    u[2] * v[0] - u[0] * v[2],
                    u[0] * v[1] - u[1] * v[0],
                )
                self.assertLess(
                    sum(normal[axis] * (center[axis] - b[axis]) for axis in range(3)),
                    0,
                )
                normals.add(tuple(0 if n == 0 else (1 if n > 0 else -1) for n in normal))
            self.assertEqual(normals, {
                (-1, 0, 0), (1, 0, 0), (0, -1, 0),
                (0, 1, 0), (0, 0, -1), (0, 0, 1),
            })
            bounds.append((low, high))
        self.assertEqual(bounds, [
            ((-272, -272, -16), (272, 272, 0)),
            ((-272, -272, 256), (272, 272, 272)),
            ((-272, -272, 0), (-256, 272, 256)),
            ((256, -272, 0), (272, 272, 256)),
            ((-256, -272, 0), (256, -256, 256)),
            ((-256, 256, 0), (256, 272, 256)),
        ])
        spawn_low, spawn_high = (-16, -16, 40), (16, 16, 104)
        for low, high in bounds:
            self.assertTrue(any(
                spawn_high[axis] <= low[axis] or spawn_low[axis] >= high[axis]
                for axis in range(3)
            ))


if __name__ == "__main__":
    unittest.main()
