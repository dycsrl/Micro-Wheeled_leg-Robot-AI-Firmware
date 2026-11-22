import sys, socket, threading, unittest, math
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from codec import *


class Protocol(unittest.TestCase):
    def sample(self):
        d = {n: [0] * c for t, n, c in FIELDS}
        d.update(magic=[0x32544252], version=[1], bytes=[STRUCT.size])
        d["input"] = list(range(63))
        return d

    def test_roundtrip(self):
        self.assertEqual(decode(encode(self.sample())), self.sample())

    def test_reject_magic(self):
        b = bytearray(encode(self.sample()))
        b[0] = 0
        with self.assertRaises(ValueError):
            decode(b)

    def test_truncated(self):
        with self.assertRaises(struct.error):
            decode(encode(self.sample())[:-1])

    def test_loopback(self):
        # 合成数据，真实TCP loopback分片传输，非实机日志。
        server = socket.socket()
        server.bind(("127.0.0.1", 0))
        server.listen(1)
        payload = encode(self.sample()) * 17

        def send():
            with server.accept()[0] as conn:
                for i in range(0, len(payload), 7):
                    conn.sendall(payload[i : i + 7])
            server.close()

        t = threading.Thread(target=send)
        t.start()
        data = b""
        with socket.create_connection(server.getsockname()) as client:
            while True:
                b = client.recv(113)
                if not b:
                    break
                data += b
        t.join()
        self.assertEqual(data, payload)
        self.assertEqual(
            len([decode(data[i : i + STRUCT.size]) for i in range(0, len(data), STRUCT.size)]), 17
        )


if __name__ == "__main__":
    unittest.main()
