const config = require("../../config/app.config")

function getGoogleOauthUrl() {
    const { googleOauthRedirectURL, googleOauthClientId } = config;
    console.log(googleOauthRedirectURL, googleOauthClientId)

    const rootUrl = 'https://accounts.google.com/o/oauth2/v2/auth';
    const scopes = [
        'https://www.googleapis.com/auth/userinfo.profile',
        'https://www.googleapis.com/auth/userinfo.email',
    ].join(' ');

    const options = {
        redirect_uri: googleOauthRedirectURL,
        client_id: googleOauthClientId,
        access_type: 'offline',
        response_type: 'code',
        prompt: 'consent',
        scope: scopes,
    };
    const queryString = new URLSearchParams(options);
    const googleOauthUrl = `${rootUrl}?${queryString.toString()}`;

    return googleOauthUrl;
}

async function getGoogleOauthTokens(googleOauthCode) {
    const { googleOauthClientId, googleOauthClientSecret, googleOauthRedirectURL } =
        config;

    const googleOauthTokenUrl = 'https://oauth2.googleapis.com/token';
    const googleOauthTokenQueryValues = {
        code: googleOauthCode,
        client_id: googleOauthClientId,
        client_secret: googleOauthClientSecret,
        redirect_uri: googleOauthRedirectURL,
        grant_type: 'authorization_code',
    };

    const queryString = new URLSearchParams(googleOauthTokenQueryValues);

    try {
        const googleOAuthTokensRequest = await fetch(`${googleOauthTokenUrl}?${queryString.toString()}`, {
            method: "POST",
            headers: {
                "Content-Type": "application/x-www-form-urlencoded"
            }
        });
        const result = await googleOAuthTokensRequest.json();
        return { error: false, data: result }
    } catch(error) {
        return { error: true, data: null}
    }
}

async function getGoogleUserData(id_token, access_token) {
    const googleUserUrl = `https://www.googleapis.com/oauth2/v1/userinfo?alt=json&access_token=${access_token}`;

    try {
        const googleUserRequest = await fetch(googleUserUrl, {
            headers: {
                "Authorization": `Bearer ${id_token}`
            }
        });
        const result = await googleUserRequest.json()
        return { error: false, data: result}
    } catch (error) {
        return { error: true, data: null }
    }
}

module.exports = { getGoogleOauthUrl, getGoogleOauthTokens, getGoogleUserData }

