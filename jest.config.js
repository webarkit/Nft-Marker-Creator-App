module.exports = {
    roots: ['<rootDir>/test'],
    testMatch: ['**/?(*.)+(spec|test).[jt]s?(x)'],
    transform: {
        '^.+\\.[tj]sx?$': 'babel-jest',
    },
};