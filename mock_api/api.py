import logging
import json

from tornado import ioloop, web

logging.basicConfig(level=logging.INFO)


class BadData(Exception):
    pass


class LightState(object):
    def __init__(self):
        self._set_off_mode()

    def set_state(self, data):
        try:
            mode = data['mode']
        except KeyError:
            raise BadData

        if mode == 'color':
            self._set_color_mode(data)
        elif mode == 'rainbow':
            self._set_rainbow_mode(data)
        elif mode == 'flash':
            self._set_flash_mode(data)
        elif mode == 'off':
            self._set_off_mode()
        else:
            raise BadData

    def _set_off_mode(self):
        self.state = {'mode': 'off'}

    def _set_color_mode(self, data):
        DEFAULTS = {
            'red': 0,
            'green': 0,
            'blue': 0,
            'white': 0,
            'brightness': 255}

        self.state = {'mode': data['mode']}

        for name, default in DEFAULTS.items():
            self.state[name] = self._get_int(data, name, default)

    def _set_rainbow_mode(self, data):
        DEFAULTS = {
            'interval': 50,
            'brightness': 255}

        self.state = {'mode': data['mode']}

        for name, default in DEFAULTS.items():
            self.state[name] = self._get_int(data, name, default)

    def _set_flash_mode(self, data):
        DEFAULTS = {
            'red': 0,
            'green': 0,
            'blue': 0,
            'white': 0,
            'interval': 50,
            'brightness': 255}

        self.state = {'mode': data['mode']}

        for name, default in DEFAULTS.items():
            self.state[name] = self._get_int(data, name, default)


    def _get_int(self, body, name, default):
        value = body.get(name, default)
        if not isinstance(value, int):
            try:
                value = int(value)
            except (ValueError, TypeError):
                raise BadData

        if value < 0 or value > 255:
            raise BadData
        return value


class LightsHandler(web.RequestHandler):
    def write(self, body):
        logging.info(f'Response: {body}')
        super().write(body)

    def get(self):
        self.write(self.application.lights.state)

    def put(self):
        body = json.loads(self.request.body)
        logging.info(f'Request: {body}')

        try:
            self.application.lights.set_state(body)
        except BadData:
            raise web.HTTPError(400)

        self.write(self.application.lights.state)


def make_app():
    app = web.Application([('/', LightsHandler)],
                          autoreload=True)

    app.lights = LightState()

    return app


app = make_app()
app.listen(8000)
ioloop.IOLoop.current().start()
