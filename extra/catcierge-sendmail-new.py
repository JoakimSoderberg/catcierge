#!/usr/bin/python
from optparse import OptionParser
import argparse
import smtplib
import json
from os.path import basename

from email import encoders
from email.mime.base import MIMEBase
from email.mime.image import MIMEImage
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
import mimetypes
from pprint import pprint

def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("--images", metavar = "IMAGES", nargs = "+", 
                    help = "The Catcierge match images to send.")

    parser.add_argument("--to", metavar = "TO", nargs = "+", required=True,
                    dest = "to_emails",
                    help = "List of E-mail addresses to send notification to")

    parser.add_argument("--from", dest = "from_email", metavar = "FROM", required=True,
                    help = "E-mail to send from")

    parser.add_argument("--smtp", metavar = "SMTP", default = "smtp.gmail.com:587",
                    help = "SMTP server to use. Defaults to gmail")

    parser.add_argument("--password", metavar = "PASSW",
                    help = "The password to use for SMTP")

    parser.add_argument("--json", metavar="PATH", required=True,
                    help="JSON containing image paths and descriptions.")

    parser.add_argument("--extra", metavar="FILES", nargs="+", default=[],
                    help="Extra files to attach.")

    args = parser.parse_args()

    print("Json: %s" % args.json)
    with open(args.json) as f:
        j = json.load(f)

    success = j["match_group_success"]
    status_str = "Ok" if success else "Prey Detected"
    state = j["state"].replace("_", "")
    direction = j["match_group_direction"]
    group_id = j["id"]
    start_date = j["match_group_start"]
    end_date = j["match_group_end"]

    # Create the container (outer) email message.
    msg = MIMEMultipart()
    
    msg['Subject'] = 'Catcierge Detection {dir}: {status} '.format(dir=direction, status=status_str)
    msg['From'] = args.from_email
    msg['To'] = ', '.join(args.to_emails)
    msg.preamble = 'Catcierge'

    img_html = ""

    for f in args.images:
        img_html += '<img src="cid:%s">' % basename(f)

    msg_html = """\
    <html>
        <head></head>
        <body>
            <h1>Detection status {status} - {state}</h1>
            <p>{imgs}</p>
            <p>
                ID: {id}<br>
                State: {state}<br>
                Start: {start}<br>
                End: {end}<br>
            </p>
        </body>
    </html>
    """.format(status=status_str, state=state, imgs=img_html,
               id=group_id, start=start_date, end=end_date)

    msg.attach(MIMEText(msg_html, 'html'))

    for f in args.images:
        # Open the files in binary mode. Let the MIMEImage class automatically
        # guess the specific image type.
        fp = open(f, 'rb')
        img = MIMEImage(fp.read())
        img.add_header('Content-ID', '<%s>' % basename(f))
        fp.close()
        msg.attach(img)

    # Attach extra files.
    for path in args.extra:
        # Guess the content type based on the file's extension.  Encoding
        # will be ignored, although we should check for simple things like
        # gzip'd or compressed files.
        ctype, encoding = mimetypes.guess_type(path)
        if ctype is None or encoding is not None:
            # No guess could be made, or the file is encoded (compressed), so
            # use a generic bag-of-bits type.
            ctype = 'application/octet-stream'
        maintype, subtype = ctype.split('/', 1)
        if maintype == 'text':
            fp = open(path)
            # Note: we should handle calculating the charset
            atchmnt = MIMEText(fp.read(), _subtype=subtype)
            fp.close()
        elif maintype == 'image':
            fp = open(path, 'rb')
            atchmnt = MIMEImage(fp.read(), _subtype=subtype)
            fp.close()
        elif maintype == 'audio':
            fp = open(path, 'rb')
            atchmnt = MIMEAudio(fp.read(), _subtype=subtype)
            fp.close()
        else:
            fp = open(path, 'rb')
            atchmnt = MIMEBase(maintype, subtype)
            atchmnt.set_payload(fp.read())
            fp.close()
            # Encode the payload using Base64
            encoders.encode_base64(atchmnt)
        # Set the filename parameter
        atchmnt.add_header('Content-Disposition', 'attachment', filename=basename(path))
        msg.attach(atchmnt)

    # Send the email via our own SMTP server.
    s = smtplib.SMTP(args.smtp)
    s.starttls()
    s.login(args.from_email, args.password)
    s.sendmail(args.from_email, args.to_emails, msg.as_string())
    s.quit()

if __name__ == '__main__': main()
