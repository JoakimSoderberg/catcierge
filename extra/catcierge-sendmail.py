#!/usr/bin/python
from optparse import OptionParser
import argparse
import smtplib

from email.mime.image import MIMEImage
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

def send_mail(to_emails, from_email, smtp_server, password, images, match_status, match_statuses, direction):
	# Create the container (outer) email message.
	msg = MIMEMultipart()
	msg['Subject'] = 'Catcierge detection %s' % ("OK" if match_status else "FAIL")
	msg['From'] = from_email
	msg['To'] = ', '.join(to_emails)
	msg.preamble = 'Catcierge'

	txt = "Match: %s\n" % ("OK" if match_status else "FAIL")

	try:
		direction = int(direction)

		if direction != -1:
			txt += "  Direction: %s\n" % ("OUT" if direction else "IN")
		else:
			txt += "  Direction: UNKNOWN"
	except Exception:
		txt += "  Direction: %s" % direction.upper()

	for m in match_statuses:
		txt += "  Result: %s\n" % m

	msg.attach(MIMEText(txt))

	for f in images:
		# Open the files in binary mode.  Let the MIMEImage class automatically
		# guess the specific image type.
		fp = open(f, 'rb')
		img = MIMEImage(fp.read())
		fp.close()
		msg.attach(img)

	# Send the email via our own SMTP server.
	s = smtplib.SMTP(smtp_server)
	s.starttls()
	s.login(from_email, password)
	s.sendmail(from_email, to_emails, msg.as_string())
	s.quit()

def main():
	parser = argparse.ArgumentParser()

	parser.add_argument("--images", metavar = "IMAGES", nargs = "+", 
					help = "The Catcierge match images to send.")

	parser.add_argument("--to", metavar = "TO", nargs = "+",
					help = "List of E-mail addresses to send notification to")

	parser.add_argument("--from", dest = "from_email", metavar = "FROM",
					help = "E-mail to send from")

	parser.add_argument("--smtp", metavar = "SMTP", default = "smtp.gmail.com:587",
					help = "SMTP server to use. Defaults to gmail")

	parser.add_argument("--password", metavar = "PASSW",
					help = "The password to use for SMTP")

	parser.add_argument("--status", metavar = "STATUS", type = int,
					help = "The status of the match, 0 = failure, 1 = success.")

	parser.add_argument("--match_statuses", metavar="MATCHSTATUS", type = float, nargs="+",
					help = "List of statuses for each match", default = [])

	parser.add_argument("--direction", metavar="DIRECTION",
					help = "0 for in, 1 for out, -1 for unknown", default = -1)

	args = parser.parse_args()
	send_mail(args.to, args.from_email, args.smtp, args.password, args.images, args.status, args.match_statuses, args.direction)

if __name__ == '__main__': main()
