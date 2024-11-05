const { exec } = require('child_process');
const path = require('path');

test('NFTMarkerCreator should process the image', done => {
    const scriptPath = path.join(__dirname, 'NFTMarkerCreator.js');
    const imagePath = path.join(__dirname, 'pinball.jpg');
    const command = `node ${scriptPath} -I ${imagePath}`;

    exec(command, (error, stdout, stderr) => {
        expect(error).toBeNull();
        expect(stderr).toBe('');
        expect(stdout).toContain('Finished marker creation!');
        done();
    });
});